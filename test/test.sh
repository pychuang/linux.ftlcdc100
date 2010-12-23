# in case we forgot to create device files
mknod /dev/fb0 c 29 0

if [ $# != 1 ] || [ ! -d $1 ]
then
	echo "Usage: $0 <pattern dir>"
	exit 1
fi

for file in $1/*
do
	if [ -f $file ]
	then
		cat $file > /dev/fb0

		sleep 1
		for yoffset in 0 20 40 60 80 100 120 140 160 180 200 220 240 0
		do
			echo "0,$yoffset" > /sys/class/graphics/fb0/pan
		done
	fi
done

