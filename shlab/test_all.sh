i=1
while [ $i -lt 10 ]
do
	make "test0$i" > "test0$i.txt"
	make "rtest0$i" > "rtest0$i.txt"
	echo "0$i"
	i=`expr $i + 1`
done

while [ $i -lt 17 ]
do
	make "test$i" > "test$i.txt"
	make "rtest$i" > "rtest$i.txt"
	echo "$i"
	i=`expr $i + 1`
done
