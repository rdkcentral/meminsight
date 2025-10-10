- Integrate [memleakutil](https://github.com/JagadheesanD/memleakutil/)/ 
- Integrate Valgrind into GitHub Workflow - Refer [PR#12](https://github.com/rdkcentral/meminsight/pull/12)
- Refactor and Unify JSON and CSV based code
- Rename Functions and Structs
- CLI Argument Parsing Resiliency

Command | Interval | Iterations | Behaviour
-- | -- | -- | --
xmeminsight | 15m | ∞ | Indefinite run with Default Time 
xmeminsight --interval 5 | 5s | ∞ |  TBU
xmeminsight --interval 0 | 15m | ∞  |  TBU
xmeminsight --interval -1 | 15m  |  ∞ |  TBU
xmeminsight --iterations 3 | 15m  |  3 |  TBU
xmeminsight --iterations 0 | 15m  |  ∞ |  TBU
xmeminsight --iterations -1 | 15m  | ∞  |  TBU
xmeminsight --interval 5 --iterations 3 | 5s  |  3 | TBU 
xmeminsight --interval 0 --iterations 3 | 15m  |  3 |  TBU
xmeminsight --interval 5 --iterations 0 | 5s  |  ∞ |  TBU
xmeminsight --interval abc --iterations 3 | 15m  |  3 |  TBU
xmeminsight --interval 5 --iterations abc |  5s | ∞  |  TBU
xmeminsight --iterations abc | 15m  |  ∞ |  TBU
xmeminsight --interval abc | 15m  | ∞  |  TBU
xmeminsight --interval 0 --iterations 0 | 15m  | ∞  |  TBU
xmeminsight --interval -1 --iterations -1 | 15m  | ∞  |  TBU
xmeminsight --interval abc --iterations xyz | 15m  | ∞  |  TBU
xmeminsight --interval 1 --iterations 1 | 1s  | 2 |  TBU
xmeminsight --interval 3600 --iterations 1 | 60m  | 2  |  TBU
xmeminsight --interval 1 --iterations 86400 |  24h | 2  |  TBU
xmeminsight --interval 999999 | 999999s  |  ∞ |  Warning
xmeminsight --iterations 999999 | 15m  | 999999  |  Warning
xmeminsight --interval 2147483647 | MAX-1  |  ∞ | TBU
xmeminsight --iterations 2147483647 | 15m  | ∞ | TBU
xmeminsight --interval " 5" | TBD  | TBD   | TBD  
xmeminsight --interval "5 " | TBD   | TBD   |  TBD 
xmeminsight --interval "05" | TBD   | TBD   |  TBD 
xmeminsight --interval "+5" | TBD   | TBD   |  TBD 
xmeminsight --interval "5.5" | TBD   | TBD   | TBD  
xmeminsight --interval "5abc" | TBD   |  TBD  |  TBD 
xmeminsight --iterations 3 --interval 5 | 5s  |  3 | TBU
xmeminsight --interval 5 --interval 10 | TBD  | TBD |  TBD 
xmeminsight --iterations 3 --iterations 5 | TBD | TBD  |  TBD 

- Code Optimizations and Organization (if any)
- Default JSON Format
- Defaults numprocs to 10
- Runtime CJson linking
- CPU Data to major top info
- Update README.md