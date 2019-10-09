all: clean s c

c: ./Client/client_1.cpp ./Client/client_2.cpp ./Client/client_3.cpp ./Client/client_4.cpp ./Client/client_5.cpp ./Client/client_6.cpp ./Client/client_7.cpp ./Client/client_8.cpp
	g++ ./Client/client_1.cpp -o ./Client/client_1
	g++ ./Client/client_2.cpp -o ./Client/client_2
	g++ ./Client/client_3.cpp -o ./Client/client_3
	g++ ./Client/client_4.cpp -o ./Client/client_4
	g++ ./Client/client_5.cpp -o ./Client/client_5
	g++ ./Client/client_6.cpp -o ./Client/client_6
	g++ ./Client/client_7.cpp -o ./Client/client_7
	g++ ./Client/client_8.cpp -o ./Client/client_8

s: ./Server/server_1.cpp ./Server/server_2.cpp ./Server/server_3.cpp ./Server/server_4.cpp ./Server/server_5.cpp ./Server/server_6.cpp ./Server/server_7.cpp ./Server/server_8.cpp
	g++ ./Server/server_1.cpp -o ./Server/server_1
	g++ ./Server/server_2.cpp -o ./Server/server_2
	g++ ./Server/server_3.cpp -o ./Server/server_3
	g++ ./Server/server_4.cpp -o ./Server/server_4
	g++ ./Server/server_5.cpp -o ./Server/server_5
	g++ ./Server/server_6.cpp -o ./Server/server_6
	g++ ./Server/server_7.cpp -o ./Server/server_7
	g++ ./Server/server_8.cpp -pthread -o ./Server/server_8

clean:
	rm -f ./Client/client_1 ./Client/client_2 ./Client/client_3 ./Client/client_4 ./Client/client_5 ./Client/client_6 ./Client/client_7 ./Client/client_8 ./Server/server_1 ./Server/server_2 ./Server/server_3 ./Server/server_4 ./Server/server_5 ./Server/server_6 ./Server/server_7 ./Server/server_8
