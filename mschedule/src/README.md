### Elif Sorguç - 22003782

### Habibe Yılmaz - 22002766

# Multithreading, Synchronization, and Scheduling

## Multi-Processor Scheduling Simulator (mschedule)

This project is a multi-processor scheduling simulator that supports two approaches: single-queue and multi-queue. It can simulate FCFS and SJF scheduling algorithms using POSIX threads in a C/Linux environment.

## Usage

#### What is included and how to run

##### mschedule.c file contains the main and all needed functions.

##### Eample inputfile.txt is included, you can try with different same format input files.

##### Makefile to crate executable.

##### to create executable file just use command:

##### _make_

##### To run the simulator, use the following command format:

##### _./mschedule -n N -a S/M QS -s FCFS/SJF [-i INFILE | -r T T1 T2 L L1 L2 PC] [-m OUTMODE]_
