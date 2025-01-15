# count_times
Program to read a text file containing timelines of a specific format and calculate how much time is spent on each activity.

## How to run
After compiling count_times.cpp, 
> ./count_times \<timeline text file\>

As a test, you can run the program with a sample timeline text in this repository example_timelines.txt.


## Input timeline format
For example,
> \- T 12:00 did task 1

This '-' represents the start of a timeline. One timeline must not contain any newline, i.e. one timeline is one-line.
Following '-', the activity type and the end time (not the start time) of the activity in the 24-hour time notation.
The list of activity types covered in this program is:
- d : travel (from 'd'rive)
- e : exercise
- h : house chore
- l : write log
- m : miscellaneous activities
- n : not_set
- p : pastime (synonym of hobby)
- r : rest
- s : social
- t : task
- w : wasteful
- z : unknown (representing an error)

The activity-type characters are case-insensitive. So you can use either 't' or 'T' for example.

The program calculates the activity duration by calculating the difference between two end times. For example,
> \- T 12:00 did task 1 (unit tests)
> 
> \- E 12:30 practiced tennis

From these two lines, the program knows that the exercise time is 30 minutes (12:30-12:00).

### Task classification digit
For the activity "task", you can also attach a task digit to differentiate different tasks, for example,
> \- T1 12:00 did task 1

If you don't attach the task digit, the program tries to deduce the task digit by scanning the activity content and looking for a string "task \d" with regex. \d part is used for the task digit.
In this example, since there is "task 1" in the text, you don't have to put the task digit after 'T'.

### Sub-activities
You can also specify sub-activities. For example,
> \- T+w 12:00 did task 1. Watched YouTube (w ~20m).

The tailing "+w" shows the sub-activity you did in the main activity. 
You need to specify the duration of the sub-activity or the time stamps by enclosing it with parentheses, e.g. (w \~20m) or (w \~11:40 - 12:00).
The program accepts both formats. 
It also accepts '\~' (meaning "approximately") before the duration or time stamps (attaching '\~' doesn't change the program's behavior. It is simply for the user's convenience).
You can attach as many sub-activities as you want.
Even if you forget to attach sub-activities in the activity list "T+w", the program searches the text for the parenthesized duration or time stamps (e.g. (w \~20m) or (w \~11:40 - 12:00)), and if it finds it, it adds the sub-activity automatically.
