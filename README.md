# count_times
Program to read a text file containing timelines of a specific format and calculate how much time is spent on each activity.

## Input timeline format
For example,
> - T 12:00 did task 1
This '-' represents the start of a timeline. One timeline must not contain any newline, i.e. one timeline is one-line.
Following '-', the activity type and the end time (not the start time) of the activity in the 24-hour time notation.
The list of activity types covered in this program is:

