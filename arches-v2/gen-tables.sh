if test -f "$1/0-0.log"; then
    ./gen-table.sh "$1" 0 > "$1-0.txt"
    ./format-table.sh "$1-0.txt" > "$1-0-table.txt"
fi

if test -f "$1/1-0.log"; then
    ./gen-table.sh $1 1 > "$1-1.txt"
    ./format-table.sh "$1-1.txt" > "$1-1-table.txt"
fi

if test -f "$1/2-0.log"; then
    ./gen-table.sh $1 2 > "$1-2.txt"
    ./format-table.sh "$1-2.txt" > "$1-2-table.txt"
fi

if test -f "$1/3-0.log"; then
    ./gen-table.sh $1 3 > "$1-3.txt"
    ./format-table.sh "$1-3.txt" > "$1-3-table.txt"
fi