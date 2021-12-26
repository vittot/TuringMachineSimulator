# TuringMachineSimulator
Turing Machine Simulator - Project of Algorithms and Data Structures @ Polimi A.Y. 2017/2018

The project is a simulator for a single tape non-deterministic Turing Machine, written in C. 
The program receives from standard input the TM graph, together with a maximum number of computation steps (to avoid unlimited computation which could happen with certain inputs) and a series of strings to process.
It produces on standard output the results for the processed strings (1 = accepted, 0=not accepted, U=computation limit reached).

The first part of the input starts with `tr` and contains a TM transition per line, in the following format:
`0 a c R 1` means it moves from state 0 to state 1, reading 'a' and writing 'c' and moving right (R) on the tape.

The second part starts with `acc` and contains the accepting states, one per line.

The third part starts with `max` and contains the maximum number of steps before producing U in output.

The fourth part starts with `run` and contains the strings to process, one per line.

The output will contain the result of each input string, one per line.
