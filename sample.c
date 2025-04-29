[+] Execution Complete
Wall-clock time:    3 seconds
User CPU time:      0.001062 seconds
System CPU time:    0.001984 seconds
Max memory used:    0.97 MB
Exit status:        0
usage: sleep number[unit] [...]
Unit can be 's' (seconds, the default), m (minutes), h (hours), or d (days).

[+] Execution Complete
Wall-clock time:    0 seconds
User CPU time:      0.000931 seconds
System CPU time:    0.000926 seconds
Max memory used:    0.97 MB
Exit status:        1
Error: No command specified
Usage: ./timedexec --time SECONDS COMMAND [ARGS...]
./timedexec_test_cases.sh: line 15: 99201 Segmentation fault: 11  ./timedexec --time 5 sleep 5
timedexec: invalid option -- 5
Try './timedexec --help' for usage
Timedexec - Run commands with time limits

Usage: ./timedexec --time SECONDS COMMAND [ARGS...]

Examples:
  ./timedexec --time 5 sleep 10   # Kills after 5 seconds
  ./timedexec --time 1 ls -l      # Lists files (max 1 second)
  ./timedexec --time 0.5 ./a.out  # Sub-second precision
./timedexec_test_cases.sh: line 24: 99205 Segmentation fault: 11  ./timedexec -t 5 sleep 5
test 1

[+] Execution Complete
Wall-clock time:    0 seconds
User CPU time:      0.000979 seconds
System CPU time:    0.001316 seconds
Max memory used:    0.97 MB
Exit status:        0
test 1

[+] Execution Complete
Wall-clock time:    0 seconds
User CPU time:      0.000870 seconds
System CPU time:    0.001023 seconds
Max memory used:    0.97 MB
Exit status:        0
(base) legacy@KodieXLegacy project % 
