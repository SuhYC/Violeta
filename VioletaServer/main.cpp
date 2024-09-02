#include <iostream>
#include <conio.h>
#include "VioletaServer.hpp"

const long _port = 50000;
const long _maxClient = 100;

int main()
{
	
	VioletaServer server;
	server.Init(_port);

	server.Run(_maxClient);

	_getch();

	server.End();
	


	return 0;
}