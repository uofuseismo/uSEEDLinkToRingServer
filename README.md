# About

seedLinkToRingServer is intended to import and monitor the import of seismic data from an external aggregation node.  To do this, seedLinkToRingServer imports seismic packets from a data source via SEEDLink then forwards those packets to local RingServer(s) via DataLink.  To faciliate our transition to MiniSEED3, packets can be forwarded as MiniSEED2 or MiniSEED3.

<img width="681" height="401" alt="seedLinkToRingServer drawio" src="https://github.com/user-attachments/assets/1e43cd1f-04ec-4c79-99fa-a00761b46046" />

During the import process, metrics are tabulated on a per-stream (Network, Station, Channel, Location Code) basis.  These metrics are exposed to a Prometheus database.  Metrics include 

  1. The number of `good' packets received per stream
  2. The number of packets containing future data per stream
  3. The number of packets containing expired data (e.g., older than 6 months) per stream
  4. The total number of packets received (this would be `good', future, and expired packets) per stream
  5. The average counts in a sampling window (e.g., 5 minutes) per stream
  6. The standard deviation of the counts in a sampling window (e.g., 6 minutes) per stream

Once made available to Prometheus, tools like Prometheus Alert manager can notify interested parties of drops in data and tools like Grafana can visualize the data collection on a per stream basis.
