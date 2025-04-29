#!/bin/bash

# Full toolkit menu: Loganalyzer (macOS), Timedex (macOS), Memview + NetstatPlus (Linux via Docker)

LOG_DEMO="demo.log"
LOG_BIG="big.log"
ANALYZER="./loganalyzer"
TIMEDEX="./timedex"
MEMVIEW_SRC="memview.c"
NETSTATPLUS_SRC="netstatplus.c"

function test_loganalyzer() {
    echo -e "\n[LogAnalyzer] Running full test suite...\n"
    $ANALYZER -f "$LOG_DEMO"
    $ANALYZER -f "$LOG_DEMO" -t 4
    $ANALYZER -f "$LOG_DEMO" -l warn
    time $ANALYZER -f "$LOG_BIG" -t 8
    $ANALYZER -f "$LOG_BIG" -t 128
    echo "✔ Done testing LogAnalyzer."
}

function test_timedex() {
    echo -e "\n[Timedex] Running tests...\n"
    $TIMEDEX --time 2 sleep 1
    $TIMEDEX --time 2 sleep 5
    $TIMEDEX --time 10 -- $ANALYZER -f "$LOG_DEMO"
    $TIMEDEX --time 0 -- echo 'Test'
    echo "✔ Done testing Timedex."
}

function run_memview_docker() {
    clear
    echo "====== Memview Options ======"
    echo "1) System memory (-s)"
    echo "2) Shared memory (-m)"
    echo "3) PID 1 verbose (-p 1 -v)"
    echo "4) All tests"
    echo "5) Back to main menu"
    echo "=============================="
    read -p "Choose an option [1-5]: " opt
    local CMD=""
    case $opt in
        1) CMD="./memview -s" ;;
        2) CMD="./memview -m" ;;
        3) CMD="./memview -p 1 -v" ;;
        4) CMD="./memview -s && ./memview -m && ./memview -p 1 -v" ;;
        5) return ;;
        *) echo "Invalid option."; return ;;
    esac

    echo -e "\n[Docker] Running memview with: $CMD\n"
    docker run --rm -it \
        --privileged \
        -v "$PWD":/src -w /src \
        gcc:13 bash -c " \
            apt-get update -qq && \
            apt-get install -y procps > /dev/null && \
            gcc -O2 -D_GNU_SOURCE -o memview memview.c && \
            echo 'Running memview...' && \
            $CMD"
}

function run_netstatplus_docker() {
    clear
    echo "====== NetstatPlus Tests ======"
    echo "1) Stats only (-s -o)"
    echo "2) TCP only (-t -o)"
    echo "3) UDP only (-u -o)"
    echo "4) Listening sockets (-l -o)"
    echo "5) All info (-t -l -s -o)"
    echo "6) Back to main menu"
    echo "================================"
    read -p "Choose an option [1-6]: " opt
    local CMD=""
    case $opt in
        1) CMD="./netstatplus -s -o" ;;
        2) CMD="./netstatplus -t -o" ;;
        3) CMD="./netstatplus -u -o" ;;
        4) CMD="./netstatplus -l -o" ;;
        5) CMD="./netstatplus -t -l -s -o" ;;
        6) return ;;
        *) echo "Invalid option."; return ;;
    esac

    echo -e "\n[Docker] Running netstatplus with: $CMD\n"
    docker run --rm -it \
        --privileged \
        --network host \
        -v "$PWD":/src -w /src \
        gcc:13 bash -c " \
            apt-get update -qq && \
            apt-get install -y procps > /dev/null && \
            gcc -O2 -D_GNU_SOURCE -o netstatplus netstatplus.c && \
            echo 'Running netstatplus...' && \
            $CMD"
}

function main_menu() {
    clear
    echo "========= Full Toolkit (LogAnalyzer + Memview + NetstatPlus + Timedex) ========="
    echo "1) Run LogAnalyzer on demo.log"
    echo "2) Run LogAnalyzer on big.log (8 threads)"
    echo "3) Run Timedex on demo.log"
    echo "4) Test Memview (via Docker)"
    echo "5) Test NetstatPlus (via Docker)"
    echo "6) Run All Tools (Docker for Memview & NetstatPlus)"
    echo "7) Run All Tests (LogAnalyzer + Timedex + Memview + NetstatPlus)"
    echo "8) Exit"
    echo "================================================================================="
    read -p "Choose an option [1-8]: " choice

    case $choice in
        1)
            echo -e "\n[LogAnalyzer] Analyzing demo.log...\n"
            $ANALYZER -f "$LOG_DEMO"
            ;;
        2)
            echo -e "\n[LogAnalyzer] Analyzing big.log with 8 threads...\n"
            $ANALYZER -f "$LOG_BIG" -t 8
            ;;
        3)
            echo -e "\n[Timedex] Running on demo.log...\n"
            $TIMEDEX "$LOG_DEMO"
            ;;
        4)
            run_memview_docker
            ;;
        5)
            run_netstatplus_docker
            ;;
        6)
            echo -e "\n[All Tools] Running LogAnalyzer, Timedex, Memview, NetstatPlus...\n"
            echo "➤ [1/4] LogAnalyzer:"
            time $ANALYZER -f "$LOG_BIG" -t 8
            echo -e "\n➤ [2/4] Timedex:"
            $TIMEDEX "$LOG_BIG"
            echo -e "\n➤ [3/4] Memview:"
            run_memview_docker
            echo -e "\n➤ [4/4] NetstatPlus:"
            run_netstatplus_docker
            ;;
        7)
            echo -e "\n🧪 Running All Tests...\n"
            test_loganalyzer
            test_timedex
            run_memview_docker
            run_netstatplus_docker
            ;;
        8)
            echo "Exiting. Goodbye!"
            exit 0
            ;;
        *)
            echo "Invalid choice."
            ;;
    esac

    echo -e "\nPress Enter to return to the menu..."
    read
    main_menu
}

main_menu


# ./full_toolkit_dockerized.sh