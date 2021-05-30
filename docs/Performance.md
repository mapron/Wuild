# Performance and benchmarking

What did you need to consider:
1. Wuild do not perform remote compilation when there is less than 10 remote tasks scheduled for current build target. That can be changed with "minimalRemoteTasks=0" in "[toolClient]".
2. WuildNinja has some performance-related output at the ond of the build. It looks like
```
sid:1620039598424798(err:0), maxThreads:38 Total remote tasks:58, done in 00:42 total compilationTime: 04:12,  total networkTime: 10:11,  total overhead: 141% sent KiB: 21329,  recieved KiB: 17310,  compression time: 00:02,
```
What you can pay attention to:  
- maxThreads  - maximum REMOTE threads/cores utilized in a peak dureng the build (do if it was like 38, adding 100 more cores probably won't do a thing - for a single compilation run ofc. for the purpose of CI server with dozens of jobs running at once still worthy).

- Total remote tasks - usually just equal to amount of all cpp files. If it is lower, than is sometimes fallback to local build (so you need to add more remote threads)

- "done in" - wall clock time, starting from a time remote server is created (can be 1-2 seconds off ninja run time, that's ok)

- overhead - that's not "REAL" overhead! I just did not imagine correct waord for it. It ratio from total roundabout time "preprocess+send to server+compile+send response" to just "compile on serverside". That does not mean you actually spending CPU ticks 200% more! . Numbers in range 20-40% is ideal - 100-200% is probably the common case for local network, and 1000% and more you can see over VPN and really bad ping (100ms+). over VPN it brings and advantage, but noticeable for 2-4 cores, and neglible for 16 cores (expect +10% speed for that case).
- sent/recieved - traffic size, in application level(what amount of bytes written to socket()). Highly recommended at least 100Mb network for test, and not Wi-Fi. (it's too unstable and not reproduceable. for benchmarks it's bad, for general usage it's ok).

- compression time, seconds. At the moment zstd compression is done in same thread ninja schedules all other tasks. so, if you seeing large numbers there compared to wall clock build time (more than 5% .e.g), please report that. That can drastically affect performance. As a workaround you can play with "compressionLevel=3" (default) in ini config. But I 99% sure that should never be the bottleneck, maybe just if you cave really really big object files and very fast network, dunno.

3. Make sure compare difference between "plain old ninja" and "WuildNinja but remotes are unavailbale" case (first step in the Quick Start). That should not be more that 5% of slowdown. (1-2% is ok as I remember my measurements).
4. If your major speed issue is a compilation, not a link time and graph is not highly interdependent, you probably should see almost linear speedup depending how many remote cores you provide. That have a dimimishing return around 80-120 cores - where actual preprocessing speed became to matter.
You now can make sure all sources and header files are on fast SSD if disk read became a bottleneck. When disk is not an issue, you having 100% cpu load on a client and "remoteThreads" is no longer grows, you will know you reached the limit for you project and CI setup.