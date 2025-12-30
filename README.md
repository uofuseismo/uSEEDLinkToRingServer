# About

seedLinkToRingServer is intended to import seismic data.  To do this, seedLinkToRingServer imports seismic packets from a data source then forwards those packets to local RingServer.  

<img width="681" height="401" alt="seedLinkToRingServer drawio" src="https://github.com/user-attachments/assets/1e43cd1f-04ec-4c79-99fa-a00761b46046" />

During the import process, metrics are tabulated on a per-stream (Network, Station, Channel, Location Code) basis.  These metrics are exposed to a Prometheus database.  Metrics include 

  1. The number of `good' packets received per stream
  2. The number of packets containing future data per stream
  3. The number of packets containing expired data (e.g., older than 6 months) per stream
  4. The total number of packets received (this would be `good', future, and expired packets) per stream
  5. The average counts in a sampling window (e.g., 5 minutes) per stream
  6. The standard deviation of the counts in a sampling window (e.g., 6 minutes) per stream
