gcc src/processA.c -lncurses -o bin/processA -lbmp -lm -lrt -lpthread
gcc src/processB.c -lncurses -o bin/processB -lbmp -lm -lrt -lpthread
gcc src/master.c -o bin/master 

./bin/master

