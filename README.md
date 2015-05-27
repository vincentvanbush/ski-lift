# ski-lift

This is a Distributed Processing class assignment project dealing with the problem of distributed mutual exclusion in a case where traditional algorithms - the likes of Lamport's or Ricart-Agrawala's solutions - are insufficient.

## Problem overview

k skiers, each of whom weighs w<sub>k</sub>, are skiing on a hill equipped with two ski lifts, each lift having a maximum capacity of c<sub>1</sub> and c<sub>2</sub>, respectively. Every skier, in an infinite loop, rides downhill for a random period of time and then goes uphill for another random time, using one of the two lifts.

Given those circumstances, the goal is to create a fully distributed algorithm which allows the skiers to use both lifts. Obviously the algorithm ought to satisfy both the safety condition (at no moment can the sum of the weights of skiers using a lift exceed the lift's maximum capacity) and the progress condition (any skier that wants to use the lift will be permitted to do it in finite time).

## Running

The program uses the PVM3 distributed computing environment. Compile the source using the Makefile provided. More instructions on compiling and running the application will follow.

Usage:
<pre>./starter [skiers_count] [weights w1 w2 w3 ...] [lifts c1 c2]</pre>

where w1, w2, w3 denote the weights of respective skiers and c1, c2 denote the weights of lifts. All arguments are optional, when not provided, the program chooses random values.
