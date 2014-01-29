#!/bin/bash

until who | grep "$1" > /dev/null
do
	sleep 60
done

#now ring the bell and announce the expected user.

echo -e '\a'
echo "*** $1 has just logged in ***"

exit0

#If the user is already logged on, the loop doesn't need to execute at all. so using until is more natural choice than while 
