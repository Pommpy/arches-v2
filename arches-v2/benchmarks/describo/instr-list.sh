grep -P -o "\t[a-z.]+($|\t)" $1 | grep -P -o "[a-z.]+" | sort | uniq