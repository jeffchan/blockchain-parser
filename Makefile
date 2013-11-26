blockchain.out: *.cpp *.h
	g++ *.cpp -o blockchain.out
run:	blockchain.out
	./blockchain.out
