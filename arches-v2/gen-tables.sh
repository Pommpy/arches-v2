if test -f "$1/bvh-0.log"; then
    ./gen-table.sh $1 bvh > "$1-bvh.txt"
fi

if test -f "$1/tt1-0.log"; then
    ./gen-table.sh $1 tt1 > "$1-tt1.txt"
fi

if test -f "$1/tt2-0.log"; then
    ./gen-table.sh $1 tt2 > "$1-tt2.txt"
fi

if test -f "$1/tt3-0.log"; then
    ./gen-table.sh $1 tt3 > "$1-tt3.txt"
fi
