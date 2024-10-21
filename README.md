# Multi-Processor Scheduling Simulator (mschedule)

This project is a multi-processor scheduling simulator that supports two approaches: single-queue and multi-queue. It can simulate FCFS and SJF scheduling algorithms using POSIX threads in a C/Linux environment.

## Usage

To run the simulator, use the following command format:
./mschedule -n N -a S/M QS -s FCFS/SJF [-i INFILE | -r T T1 T2 L L1 L2 PC] [-m OUTMODE]
