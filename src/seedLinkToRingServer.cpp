#include <iostream>
#include <mutex>
#include <condition_variable>
#include <future>
#include <atomic>
#include <csignal>
#include <filesystem>
#include <functional>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#ifdef USE_TBB
#include <oneapi/tbb/concurrent_queue.h>
#else
#include <concurrentqueue.h>
#endif
#include "uSEEDLinkToRingServer/dataLinkClient.hpp"
#include "uSEEDLinkToRingServer/dataLinkClientOptions.hpp"
#include "uSEEDLinkToRingServer/seedLinkClient.hpp"
#include "uSEEDLinkToRingServer/seedLinkClientOptions.hpp"
#include "uSEEDLinkToRingServer/packet.hpp"
#include "uSEEDLinkToRingServer/streamIdentifier.hpp"
#include "uSEEDLinkToRingServer/streamSelector.hpp"
#include "programOptions.hpp"
#include "streamMetrics.hpp"
#include "logger.hpp"
#include "metricsExporter.hpp"


std::atomic<bool> mInterrupted{false};


namespace 
{

//void setVerbosityForSPDLOG(int, spdlog::logger *logger);
std::pair<std::string, bool> parseCommandLineOptions(int argc, char *argv[]);


class Process
{
public:
    explicit Process(const ::ProgramOptions &options,
                     std::shared_ptr<spdlog::logger> logger) :
        mOptions(options),
        mLogger(logger)
    {
        if (mLogger == nullptr)
        {
            mLogger = spdlog::stdout_color_mt("ProcessConsole");
        }
        mImportQueueMaximumSize = options.importQueueSize;
        if (mOptions.exportMetrics)
        {
             SPDLOG_LOGGER_INFO(mLogger, "Initializing metrics");
             ::initializeImportMetrics(mOptions);
        }
#ifdef USE_TBB
        mImportQueue.set_capacity(mImportQueueMaximumSize);
#else
        mImportQueue
            = std::make_unique
              <
                  moodycamel::ConcurrentQueue<USEEDLinkToRingServer::Packet>
              >
              (mImportQueueMaximumSize);
#endif
        if (mOptions.dataLinkClientOptions.empty())
        {
            throw std::invalid_argument("No writers configured");
        } 
        for (auto &dataLinkClientOptions : mOptions.dataLinkClientOptions)
        {
            auto dataLinkClient
                = std::make_unique<USEEDLinkToRingServer::DataLinkClient>
                  (dataLinkClientOptions, mLogger);
            mDataLinkClients.push_back(std::move(dataLinkClient));
        }
        /*
        for (auto &seedLinkWriterOptions : mOptions.seedLinkWriterOptions)
        {
            auto seedLinkWriter
                = std::make_unique<USEEDLinkToRingServer::SEEDLinkWriter>
                   (seedLinkWriterOptions, mLogger);
            mSEEDLinkWriters.push_back(std::move(seedLinkWriter));
        }
        */
        mSEEDLinkClient
            = std::make_unique<USEEDLinkToRingServer::SEEDLinkClient>
              (mAddPacketCallbackFunction,
               mOptions.seedLinkClientOptions,
               mLogger);

    }
    /// Destructor
    ~Process()
    {
        stop();
    }
    /// Starts the processes
    void start()
    {
        stop();
        mKeepRunning = true;
        mMetricsThread = std::thread(&::Process::tabulateMetrics, this);
        // Start the writers
        mDataLinkClientFutures.clear();
        for (auto &dataLinkClient : mDataLinkClients)
        {
            mDataLinkClientFutures.push_back(dataLinkClient->start());
        }
        /*
        mSEEDLinkWriterFutures.clear();
        for (auto &seedLinkWriter : mSEEDLinkWriters)
        {
            mSEEDLinkWriterFutures.push_back(seedLinkWriter->start());
        }
        */
        // Then the reader
        mSEEDLinkClientFuture = mSEEDLinkClient->start();
    }
    /// Stops the processes
    void stop()
    {
        mKeepRunning = false;
        if (mMetricsThread.joinable()){mMetricsThread.join();}
        // Stop acquiring and give writers a chance to clear 
        if (mSEEDLinkClient){mSEEDLinkClient->stop();}
        std::this_thread::sleep_for(std::chrono::milliseconds {15});
        // Stop the writers
        for (auto &dataLinkClient : mDataLinkClients)
        {
            if (dataLinkClient){dataLinkClient->stop();}
        }
        /*
        for (auto &seedLinkWriter : mSEEDLinkWriters)
        {
            if (seedLinkWriter){seedLinkWriter->stop();}
        }
        */
        // Futures
        if (mSEEDLinkClientFuture.valid()){mSEEDLinkClientFuture.get();}
        for (auto &dataLinkClientFuture : mDataLinkClientFutures)
        {
            if (dataLinkClientFuture.valid()){dataLinkClientFuture.get();}
        }
        /*
        for (auto &seedLinkWriterFuture : mSEEDLinkWriterFutures)
        {
            if (seedLinkWriterFuture.valid()){seedLinkWriterFuture.get();}
        }
        */
    }
    /// This callback enables the SEEDLink client to add packets to be processed
    void addPacketCallback(USEEDLinkToRingServer::Packet &&packet)
    {
        try
        {
#ifdef USE_TBB
            auto approximateQueueSize = mImportQueue.size();
#else
            auto approximateQueueSize = mImportQueue->size_approx();
#endif
            if (approximateQueueSize >= mImportQueueMaximumSize)
            {
                SPDLOG_LOGGER_WARN(mLogger,
                                   "Popping elements from import queue");
#ifdef USE_TBB
                while (mImportQueue.size() >= mImportQueueMaximumSize)
#else
                while (mImportQueue->size_approx() >= mImportQueueMaximumSize)
#endif
                {
                     mImportPacketsPopped.fetch_add(1);
                     USEEDLinkToRingServer::Packet workSpace;
#ifdef USE_TBB
                     if (!mImportQueue.try_pop(workSpace))
#else
                     if (!mImportQueue->try_dequeue(workSpace))
#endif
                     {
                         SPDLOG_LOGGER_WARN(mLogger,
                             "Failed to pop element from import queue");
                         break;
                     }
                }   
            }
#ifdef USE_TBB
            if (!mImportQueue.try_push(std::move(packet)))
#else
            if (!mImportQueue->try_enqueue(std::move(packet)))
#endif
            {
                mImportPacketsFailedToEnqueue.fetch_add(1);
                SPDLOG_LOGGER_WARN(mLogger,
                    "Failed to add packet to import queue");
            }
/*
            if (!mImportQueue->try_enqueue(std::move(packet)))
            {
                mImportPacketsFailedToEnqueue.fetch_add(1);
                SPDLOG_LOGGER_WARN(mLogger,
                   "Failed to add packet to import queue");
            }
*/
        }
        catch (const std::exception &e)
        {
            SPDLOG_LOGGER_WARN(mLogger,
                "Failed to add packet to metrics queue");
        }
    }
    /// This function tabulates the metrics on the incoming packets
    void tabulateMetrics()
    {
        ::MetricsMap metricsMap; 
        //std::chrono::hours cleanMetricsInterval{2};
        constexpr std::chrono::milliseconds timeOut{25};
#ifndef NDEBUG
        //assert(!(mDataLinkClients.empty() && mSEEDLinkWriters.empty()));
        assert(!mDataLinkClients.empty());
#endif
        auto movePacket = mDataLinkClients.size() == 1 ? true : false;
        //                + mSEEDLinkWriters.size() == 1 ? true : false;
        while (mKeepRunning.load())
        {
            // Periodically tabulate the latest metrics.  Sometimes a 
            // channel will blink out so it doesn't make sense to do this
            // in the update function.  Note, the class handles the timing
            // so this is safe to repeatedly run.
            if (mOptions.exportMetrics)
            {
                metricsMap.tabulateAndResetAllMetrics();
            }
            // Update the metrics and propagate the packet
            USEEDLinkToRingServer::Packet packet;
#ifdef USE_TBB
            if (mImportQueue.try_pop(packet))
#else
            if (mImportQueue->try_dequeue(packet))
#endif
            {
                // Update metrics
                if (mOptions.exportMetrics)
                {
                    try
                    {
                        metricsMap.update(packet, mLogger);
                    }
                    catch (const std::exception &e)
                    {
                        SPDLOG_LOGGER_WARN(mLogger,
                            "Failed to update metrics for packet because {}",
                            std::string {e.what()});
                    }
                }
                // Propagate
                for (auto &dataLinkClient : mDataLinkClients)
                {
                    try
                    {
                        if (movePacket)
                        {
                            dataLinkClient->enqueue(std::move(packet));
                        }
                        else
                        {
                            dataLinkClient->enqueue(packet);
                        }
                    }
                    catch (const std::exception &e)
                    {
                        SPDLOG_LOGGER_WARN(mLogger,
                           "Failed to enqueue packet to DataLink for publishing because {}",
                           std::string {e.what()});
                    }
                }
                /*
                for (auto &seedLinkWriter : mSEEDLinkWriters)
                {
                    try
                    {
                        if (movePacket)
                        {
                            seedLinkWriter->enqueue(std::move(packet));
                        }
                        else
                        {
                            seedLinkWriter->enqueue(packet);
                        }
                    }
                    catch (const std::exception &e)
                    {
                        SPDLOG_LOGGER_WARN(mLogger,
                          "Failed to enqueue packet to SEEDLink for publishing because {}",
                           std::string {e.what()});
                    }
                }
                */
            }
            else
            {
                std::this_thread::sleep_for(timeOut);
            }
        } 
    }
    /// True indicates the all the processes are running a-okay.
    [[nodiscard]] bool checkFuturesOkay(const std::chrono::milliseconds &timeOut)
    {
        bool isOkay{true};
        try
        {
            auto status = mSEEDLinkClientFuture.wait_for(timeOut);
            if (status == std::future_status::ready)
            {
                mSEEDLinkClientFuture.get();
            }
        }
        catch (const std::exception &e)
        {
            SPDLOG_LOGGER_CRITICAL(mLogger,
                                   "Fatal error in SEEDLink import: {}",
                                   std::string {e.what()});
            isOkay = false;
        }
        try
        {
            for (auto &dataLinkClientFuture : mDataLinkClientFutures)
            {
                auto status = dataLinkClientFuture.wait_for(timeOut);
                if (status == std::future_status::ready)
                {
                    dataLinkClientFuture.get();
                }
            }
        }
        catch (const std::exception &e)
        {
            SPDLOG_LOGGER_CRITICAL(mLogger, 
                                   "Fatal error in DataLink export: {}",
                                   std::string {e.what()});
            isOkay = false;
        }
        /*
        try
        {
            for (auto &seedLinkWriterFuture : mSEEDLinkWriterFutures)
            {
                auto status = seedLinkWriterFuture.wait_for(timeOut);
                if (status == std::future_status::ready)
                {
                    seedLinkWriterFuture.get();
                }
            }
        }
        catch (const std::exception &e)
        {
            SPDLOG_LOGGER_CRITICAL(mLogger,
                                   "Fatal error in SEEDLink export: {}",
                                   std::string {e.what()});
            isOkay = false;
        }
        */
        return isOkay;
    }
    void handleMainThread()
    {
        SPDLOG_LOGGER_DEBUG(mLogger, "Main thread entering waiting loop");
        catchSignals();
        {
            while (!mStopRequested)
            {
                if (mInterrupted)
                {
                    SPDLOG_LOGGER_INFO(mLogger,
                                       "SIGINT/SIGTERM signal received!");
                    mStopRequested = true;
                    break;
                }
                if (!checkFuturesOkay(std::chrono::milliseconds {5}))
                {
                    SPDLOG_LOGGER_CRITICAL(mLogger,
                       "Futures exception caught; terminating app");
                    mStopRequested = true;
                    break;
                }
                std::unique_lock<std::mutex> lock(mStopMutex);
                mStopCondition.wait_for(lock,
                                        std::chrono::milliseconds {100},
                                        [this]
                                        {
                                              return mStopRequested;
                                        });
                lock.unlock();
            }
        }
        if (mStopRequested)
        {
            SPDLOG_LOGGER_DEBUG(mLogger, "Stop request received.  Exiting...");
            stop(); 
        }
    }
    /// Handles sigterm and sigint
    static void signalHandler(const int )
    {   
        mInterrupted = true;
    }
    static void catchSignals()
    {   
        struct sigaction action;
        action.sa_handler = signalHandler;
        action.sa_flags = 0;
        sigemptyset(&action.sa_mask);
        sigaction(SIGINT,  &action, NULL);
        sigaction(SIGTERM, &action, NULL);
    }   
//private:
    ::ProgramOptions mOptions;
    std::shared_ptr<spdlog::logger> mLogger{nullptr};    
    mutable std::future<void> mSEEDLinkClientFuture;
    mutable std::vector<std::future<void>> mDataLinkClientFutures;
    //mutable std::vector<std::future<void>> mSEEDLinkWriterFutures;
    mutable std::mutex mStopMutex;
#ifdef USE_TBB
    oneapi::tbb::concurrent_bounded_queue
    <
          USEEDLinkToRingServer::Packet
    > mImportQueue;
#else
    std::unique_ptr<moodycamel::ConcurrentQueue<USEEDLinkToRingServer::Packet>>
        mImportQueue{nullptr};
#endif
    std::thread mMetricsThread;
    std::condition_variable mStopCondition;
    std::vector<std::unique_ptr<USEEDLinkToRingServer::DataLinkClient>>
        mDataLinkClients;
    //std::vector<std::unique_ptr<USEEDLinkToRingServer::SEEDLinkWriter>>
    //    mSEEDLinkWriters;
    std::unique_ptr<USEEDLinkToRingServer::SEEDLinkClient> mSEEDLinkClient{nullptr};
    std::function<void(USEEDLinkToRingServer::Packet &&)>
        mAddPacketCallbackFunction
    {
        std::bind(&::Process::addPacketCallback, this,
                  std::placeholders::_1)
    };
    std::future<void> mDataLinkWriterFuture;
    std::atomic<uint64_t> mImportPacketsPopped{0};
    std::atomic<uint64_t> mImportPacketsFailedToEnqueue{0};
    std::atomic<bool> mKeepRunning{true};
    int mImportQueueMaximumSize{8192};
    bool mStopRequested{false};
};

}


int main(int argc, char *argv[])
{
    // Get the ini file from the command line
    std::filesystem::path iniFile;
    try
    {
        auto [iniFileName, isHelp] = ::parseCommandLineOptions(argc, argv);
        if (isHelp){return EXIT_SUCCESS;}
        iniFile = iniFileName;
    }
    catch (const std::exception &e)
    {
        spdlog::error(e.what());
        return EXIT_FAILURE;
    }

    // Read the program properties
    ::ProgramOptions programOptions;
    try
    {
        programOptions = ::parseIniFile(iniFile);
    }
    catch (const std::exception &e)
    {
        spdlog::critical(e.what());
        return EXIT_FAILURE;
    }
    constexpr int overwrite{1};
    setenv("OTEL_SERVICE_NAME",
           programOptions.applicationName.c_str(),
           overwrite);

    auto logger = ::initializeLogger(programOptions);
    //::setVerbosityForSPDLOG(programOptions.verbosity, &*logger);

    // Setup metrics
    try
    {
        if (programOptions.exportMetrics)
        {
            SPDLOG_LOGGER_INFO(logger,
                               "Configuring OpenTelmetry metrics provider");
        }
        ::initializeMetrics(programOptions);
    }   
    catch (const std::exception &e) 
    {
        SPDLOG_LOGGER_CRITICAL(logger,
            "Failed to initialize metrics because {}",
            std::string {e.what()});
        ::cleanupLogger();
        return EXIT_FAILURE;
    }


    std::unique_ptr<::Process> process;
    try
    {
        process = std::make_unique<::Process> (programOptions, logger);
    } 
    catch (const std::exception &e)
    {
        spdlog::critical(e.what());
        ::cleanupMetrics();
        ::cleanupLogger();
        return EXIT_FAILURE;
    }

    try
    {
        SPDLOG_LOGGER_INFO(logger,
                           "Starting seedLinkToRingServer processes...");
        process->start();
        process->handleMainThread();
        if (programOptions.exportMetrics){::cleanupMetrics();}
        if (programOptions.exportLogs){::cleanupLogger();}
    }
    catch (const std::exception &e)
    {
        SPDLOG_LOGGER_CRITICAL(logger,
            "seedLinkToRingServer processes failed with {}",
            std::string {e.what()});
        if (programOptions.exportMetrics){::cleanupMetrics();}
        if (programOptions.exportLogs){::cleanupLogger();}
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

namespace
{

/*
void setVerbosityForSPDLOG(const int verbosity,
                           spdlog::logger *logger)
{
#ifndef NDEBUG
    assert(logger != nullptr);
#endif
    if (verbosity <= 1)
    {
        logger->set_level(spdlog::level::critical);
    }   
    if (verbosity == 2){logger->set_level(spdlog::level::warn);}
    if (verbosity == 3){logger->set_level(spdlog::level::info);}
    if (verbosity >= 4){logger->set_level(spdlog::level::debug);}
}   
*/

/// Read the program options from the command line
std::pair<std::string, bool> parseCommandLineOptions(int argc, char *argv[])
{
    std::string iniFile;
    boost::program_options::options_description desc(R"""(
The seedLinkToRingServer scrapes all from a SEEDLink import then forwards
those packets to RingServer(s) via DataLink.  Example usage:

    seedLinkToRingServer --ini=slinkToRing.ini

Allowed options)""");
    desc.add_options()
        ("help", "Produces this help message")
        ("ini",  boost::program_options::value<std::string> (), 
                 "The initialization file for this executable");
    boost::program_options::variables_map vm; 
    boost::program_options::store(
        boost::program_options::parse_command_line(argc, argv, desc), vm);
    boost::program_options::notify(vm);
    if (vm.count("help"))
    {
        std::cout << desc << std::endl;
        return {iniFile, true};
    }
    if (vm.count("ini"))
    {
        iniFile = vm["ini"].as<std::string>();
        if (!std::filesystem::exists(iniFile))
        {
            throw std::runtime_error("Initialization file: " + iniFile
                                   + " does not exist");
        }
    }
    return {iniFile, false};
}

USEEDLinkToRingServer::DataLinkClientOptions
getDataLinkOptions(const boost::property_tree::ptree &propertyTree,
                   const std::string &sectionName,
                   const std::string &defaultDataLinkWriterName)
{
    USEEDLinkToRingServer::DataLinkClientOptions dataLinkClientOptions;
    auto dataLinkHost
        = propertyTree.get<std::string> (sectionName + ".host",
                                         dataLinkClientOptions.getHost());
    dataLinkClientOptions.setHost(dataLinkHost);
    auto dataLinkPort
        = propertyTree.get<uint16_t> (sectionName + ".port",
                                      dataLinkClientOptions.getPort());
    dataLinkClientOptions.setPort(dataLinkPort);

    auto writeMiniSEED3
        = propertyTree.get<bool> (sectionName + ".writeMiniSEED3",
                                  dataLinkClientOptions.writeMiniSEED3());
    if (writeMiniSEED3)
    {   
        dataLinkClientOptions.enableWriteMiniSEED3();
    }   
    else
    {   
        dataLinkClientOptions.disableWriteMiniSEED3();
    }   
    auto dataLinkWriterName
        = propertyTree.get<std::string> (sectionName + ".name",
                                         defaultDataLinkWriterName);
    dataLinkClientOptions.setName(dataLinkWriterName);

    return dataLinkClientOptions;
}

/*
USEEDLinkToRingServer::SEEDLinkWriterOptions
getSEEDLinkWriterOptions(const boost::property_tree::ptree &propertyTree,
                         const std::string &sectionName,
                         const std::string &defaultSEEDLinkWriterName)
{
    USEEDLinkToRingServer::SEEDLinkWriterOptions seedLinkWriterOptions;
    auto seedLinkHost
        = propertyTree.get<std::string> (sectionName + ".host",
                                         seedLinkWriterOptions.getHost());
    seedLinkWriterOptions.setHost(seedLinkHost);
    auto seedLinkPort
        = propertyTree.get<uint16_t> (sectionName + ".port",
                                      seedLinkWriterOptions.getPort());
    seedLinkWriterOptions.setPort(seedLinkPort);

    auto writeMiniSEED3
        = propertyTree.get<bool> (sectionName + ".writeMiniSEED3",
                                  seedLinkWriterOptions.writeMiniSEED3());
    if (writeMiniSEED3)
    {   
        seedLinkWriterOptions.enableWriteMiniSEED3();
    }   
    else
    {   
        seedLinkWriterOptions.disableWriteMiniSEED3();
    }   
    auto seedLinkWriterName
        = propertyTree.get<std::string> (sectionName + ".name",
                                         defaultSEEDLinkWriterName);
    seedLinkWriterOptions.setName(seedLinkWriterName);

    return seedLinkWriterOptions;
}
*/

USEEDLinkToRingServer::SEEDLinkClientOptions
getSEEDLinkOptions(const boost::property_tree::ptree &propertyTree,
                   const std::string &clientName)
{
    USEEDLinkToRingServer::SEEDLinkClientOptions clientOptions;
    auto host = propertyTree.get<std::string> (clientName + ".host");
    auto port = propertyTree.get<uint16_t> (clientName + ".port", 18000);
    clientOptions.setHost(host);
    clientOptions.setPort(port);
    auto stateFile
        = propertyTree.get<std::string> (clientName + ".stateFile", "");
    if (!stateFile.empty())
    {
        std::filesystem::path stateFilePath{stateFile};
        if (stateFilePath.has_parent_path())
        {
            auto parentPath = stateFilePath.parent_path();
            if (!std::filesystem::exists(parentPath))
            {
                if (!std::filesystem::create_directories(parentPath))
                {
                    spdlog::warn("Could not create parent path "
                               + parentPath.string());
                }
            }
        }
        auto deleteStateFileOnStart = clientOptions.deleteStateFileOnStart();
        deleteStateFileOnStart
            = propertyTree.get<bool> (clientName + ".deleteStateFileOnStart",
                                      deleteStateFileOnStart);
        if (deleteStateFileOnStart)
        {
            clientOptions.enableDeleteStateFileOnStart();
        }
        else
        {
            clientOptions.disableDeleteStateFileOnStart();
        }

        auto deleteStateFileOnStop = clientOptions.deleteStateFileOnStop();
        deleteStateFileOnStop
            = propertyTree.get<bool> (clientName + ".deleteStateFileOnStop",
                                      deleteStateFileOnStop);
        if (deleteStateFileOnStop)
        {
            clientOptions.enableDeleteStateFileOnStop();
        }
        else
        {
            clientOptions.disableDeleteStateFileOnStop();
        }   
    }

    for (int iSelector = 1; iSelector <= 32768; ++iSelector)
    {
        std::string selectorName{clientName
                               + ".data_selector_"
                               + std::to_string(iSelector)};
        auto selectorString
            = propertyTree.get_optional<std::string> (selectorName);
        if (selectorString)
        {
            std::vector<std::string> splitSelectors;
            boost::split(splitSelectors, *selectorString,
                         boost::is_any_of(",|"));
            // A selector string can look like:
            // UU.FORK.HH?.01 | UU.CTU.EN?.01 | ....
            for (const auto &thisSplitSelector : splitSelectors)
            {
                /*
                std::vector<std::string> thisSelector;
                auto splitSelector = thisSplitSelector;
                boost::algorithm::trim(splitSelector);

                // Need to preprocess selector so there's no double spaces
                for (int k = 1; k < static_cast<int> (splitSelector.size()); )
                {
                    if (splitSelector[k - 1] == splitSelector[k] &&
                        splitSelector[k] == ' ' )
                    {
                        splitSelector.erase(k, 1);
                    }
                    else
                    {
                        ++k;
                    }
                }

                boost::split(thisSelector, splitSelector,
                             boost::is_any_of(" \t"));
                USEEDLinkToRingServer::StreamSelector selector;
                if (splitSelector.empty())
                {
                    throw std::invalid_argument("Empty selector");
                }
                // Require a network
                auto network = thisSelector.at(0);
                boost::algorithm::trim(network);
                selector.setNetwork(network);
                // Add a station?
                if (splitSelector.size() > 1)
                {
                    auto station = thisSelector.at(1);
                    boost::algorithm::trim(station);
                    selector.setStation(station);
                }
                // Add channel + location code + data type
                std::string channel{"*"};
                std::string locationCode{"??"};
                if (splitSelector.size() > 2)
                {
                    channel = thisSelector[2];
                    boost::algorithm::trim(channel);
                }
                if (splitSelector.size() > 3)
                {
                    locationCode = thisSelector[3];
                    boost::algorithm::trim(locationCode);
                }
                // Data type
                auto dataType
                    = USEEDLinkToRingServer::StreamSelector::Type::All;
                if (thisSelector.size() > 4)
                {
                    boost::algorithm::trim(thisSelector[4]);
                    if (thisSelector[4] == "D")
                    {
                        dataType = USEEDLinkToRingServer::StreamSelector::Type::Data;
                    }
                    else if (thisSelector[4] == "A")
                    {
                        dataType = USEEDLinkToRingServer::StreamSelector::Type::All;
                    }
                    // TODO other data types
                }
                selector.setSelector(channel, locationCode, dataType);
                */
                auto selector
                    = USEEDLinkToRingServer::StreamSelector::fromString(
                        thisSplitSelector);
                clientOptions.addStreamSelector(selector);
            } // Loop on selectors
        }
    }
    return clientOptions;
}

std::string getOTelCollectorURL(boost::property_tree::ptree &propertyTree,
                                const std::string &section)
{
    std::string result;
    std::string otelCollectorHost 
        = propertyTree.get<std::string> (section + ".host", "");
    uint16_t otelCollectorPort
        = propertyTree.get<uint16_t> (section + ".port", 4218);
    if (!otelCollectorHost.empty())
    {   
        result = otelCollectorHost + ":" 
               + std::to_string(otelCollectorPort);
    }   
    return result; 
}


::ProgramOptions parseIniFile(const std::filesystem::path &iniFile)
{
    ::ProgramOptions options;
    if (!std::filesystem::exists(iniFile)){return options;}
    // Parse the initialization file
    boost::property_tree::ptree propertyTree;
    boost::property_tree::ini_parser::read_ini(iniFile, propertyTree);

    // Application name
    options.applicationName
        = propertyTree.get<std::string> ("General.applicationName",
                                         options.applicationName);
    if (options.applicationName.empty())
    {
        options.applicationName = APPLICATION_NAME;
    }
    options.verbosity
        = propertyTree.get<int> ("General.verbosity", options.verbosity);

    // Metrics
    OTelHTTPMetricsOptions metricsOptions;
    metricsOptions.url
         = ::getOTelCollectorURL(propertyTree, "OTelHTTPMetricsOptions");
    metricsOptions.suffix
         = propertyTree.get<std::string> ("OTelHTTPMetricsOptions.suffix",
                                          "/v1/metrics");
    if (!metricsOptions.url.empty())
    {
        if (!metricsOptions.suffix.empty())
        {
            if (!metricsOptions.url.ends_with("/") &&
                !metricsOptions.suffix.starts_with("/"))
            {
                metricsOptions.suffix = "/" + metricsOptions.suffix;
            }
         }
    }
    if (!metricsOptions.url.empty())
    {
        options.exportMetrics = true;
        options.otelHTTPMetricsOptions = metricsOptions;
    }

    OTelHTTPLogOptions logOptions;
    logOptions.url
         = ::getOTelCollectorURL(propertyTree, "OTelHTTPLogOptions");
    logOptions.suffix
         = propertyTree.get<std::string>
           ("OTelHTTPLogOptions.suffix", "/v1/logs");
    if (!logOptions.url.empty())
    {
        if (!logOptions.suffix.empty())
        {
            if (!logOptions.url.ends_with("/") &&
                !logOptions.suffix.starts_with("/"))
            {
                logOptions.suffix = "/" + logOptions.suffix;
            }
        }
    }
    if (!logOptions.url.empty())
    {
        options.exportLogs = true;
        options.otelHTTPLogOptions = logOptions;
    }

/*
    // Prometheus
    uint16_t prometheusPort
        = propertyTree.get<uint16_t> ("Prometheus.port", 9200);
    std::string prometheusHost
        = propertyTree.get<std::string> ("Prometheus.host", "localhost");
    if (!prometheusHost.empty())
    {
        options.prometheusURL = prometheusHost + ":"
                              + std::to_string(prometheusPort);
    }
*/
/*
    // SEEDLink Writer
    std::vector<USEEDLinkToRingServer::SEEDLinkWriterOptions>
        seedLinkWriterOptions;
    if (propertyTree.get_optional<std::string> ("SEEDLinkWriter.host"))
    {
        auto seedLinkWriterName = options.applicationName + "-SEEDLinkWriter";
        seedLinkWriterOptions.push_back
        (
           getSEEDLinkWriterOptions(propertyTree, "SEEDLinkWriter", 
                                   seedLinkWriterName)
        );
    }
    else
    {
        for (int i = 1; i < 32768; ++i)
        {
            auto seedLinkSection = "SEEDLinkWriter_" + std::to_string(i);
            auto seedLinkWriterName = options.applicationName
                                    + "-SEEDLinkWriter-" + std::to_string(i);
            if (propertyTree.get_optional<std::string>
                (seedLinkSection + ".host"))
            {
                seedLinkWriterOptions.push_back(
                   getSEEDLinkWriterOptions(propertyTree,
                                            seedLinkSection,
                                            seedLinkWriterName));
            }
            else
            {
                break;
            }
        }
    }
    options.seedLinkWriterOptions = seedLinkWriterOptions;
*/

    // DataLink Writer
    std::vector<USEEDLinkToRingServer::DataLinkClientOptions>
        dataLinkClientOptions;
    if (propertyTree.get_optional<std::string> ("DataLink.host"))
    {
        auto dataLinkWriterName = options.applicationName + "-DALIWriter";
        dataLinkClientOptions.push_back
        (
           getDataLinkOptions(propertyTree, "DataLink", dataLinkWriterName)
        );
    }
    else
    {
        for (int i = 1; i < 32768; ++i)
        {
            auto dataLinkSection = "DataLink_" + std::to_string(i);
            auto dataLinkWriterName = options.applicationName
                                    + "-DALIWriter-" + std::to_string(i);
            if (propertyTree.get_optional<std::string>
                (dataLinkSection + ".host"))
            {
                dataLinkClientOptions.push_back(
                   getDataLinkOptions(propertyTree,
                                      dataLinkSection,
                                      dataLinkWriterName));
            }
            else
            {
                break;
            }
        }
    }
    options.dataLinkClientOptions = dataLinkClientOptions;

    if (options.dataLinkClientOptions.empty())
    {
        throw std::runtime_error("No data writer options specified");
    }
/*
    USEEDLinkImport::DataLinkClientOptions dataLinkClientOptions;
    auto dataLinkHost
        = propertyTree.get<std::string> ("DataLink.host",
                                         dataLinkClientOptions.getHost());
    dataLinkClientOptions.setHost(dataLinkHost);
    auto dataLinkPort
        = propertyTree.get<uint16_t> ("DataLink.port",
                                      dataLinkClientOptions.getPort());
    dataLinkClientOptions.setPort(dataLinkPort);
    options.dataLinkClientOptions = dataLinkClientOptions;

    auto writeMiniSEED3
        = propertyTree.get<bool> ("DataLink.writeMiniSEED3",
                                  dataLinkClientOptions.writeMiniSEED3());
    if (writeMiniSEED3)
    {
        dataLinkClientOptions.enableWriteMiniSEED3();
    }
    else
    {
        dataLinkClientOptions.disableWriteMiniSEED3();
    }
    auto dataLinkWriterName = options.applicationName + "-DALIWriter";
    dataLinkWriterName
        = propertyTree.get<std::string> ("DataLink.name",
                                         dataLinkWriterName);
    dataLinkClientOptions.setName(dataLinkWriterName);
 
    options.dataLinkClientOptions = dataLinkClientOptions;
*/

    // SEEDLink
    if (propertyTree.get_optional<std::string> ("SEEDLinkReader.host"))
    {
        options.seedLinkClientOptions
             = ::getSEEDLinkOptions(propertyTree, "SEEDLinkReader");
    }   

    return options;
}

}

