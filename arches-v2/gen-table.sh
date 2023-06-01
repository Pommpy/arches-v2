printf "$2\n"

printf "Total L1\n"
grep -r 'L1 Total:' $1 | grep "/$2" | grep -Eo '[0-9]{2,32}'
printf "\n"

printf "Total L2\n"
grep -r 'L2 Total:' $1 | grep "/$2" | grep -Eo '[0-9]{2,32}'
printf "\n"

printf "Total MM\n"
grep -r 'MM Total:' $1 | grep "/$2" | grep -Eo '[0-9]{2,32}'
printf "\n"

printf "L1 Energy uJ\n"
grep -r 'L1 Energy:' $1 | grep "/$2" | grep -Eo '[0-9]{2,9}'
printf "\n"

printf "L2 Energy uJ\n"
grep -r 'L2 Energy:' $1 | grep "/$2" | grep -Eo '[0-9]{2,9}'
printf "\n"

printf "MM Energy uJ\n"
grep -r 'MM Energy:' $1 | grep "/$2" | grep -Eo '[0-9]{2,9}'
printf "\n"

printf "Total Energy uJ\n"
grep -r 'Total Energy:' $1 | grep "/$2" | grep -Eo '[0-9]{2,9}'
printf "\n"

printf "Cycles\n"
grep -r 'Cycles:' $1 | grep "/$2" | grep -Eo '[0-9]{2,9}'
printf "\n"