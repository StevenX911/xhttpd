src = ${wildcard ./src/*.c}
obj = ${patsubst %.c,%.o,${src}}
clude = ./include
target = xhttpd
${target}:${obj}
	gcc ${obj} -I ${clude} -o ${target}

%.o:%.c
	gcc -c $< -I ${clude} -o $@