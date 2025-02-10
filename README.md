Word Puzzle Game with Web Interface

A C-based word game with a local web server.

Overview:

This is a word puzzle game where players must find all possible words that can be created from a randomly selected master word.
The game runs on a custom C server that handles HTTP requests.
Players interact through a web-based interface.

Features:

Dictionary-based word generation.
String manipulation algorithms for word validation.
Multi-threaded server using POSIX sockets.
Simple web-based interface to play via a browser.

How It Works:

The server picks a random word from the dictionary.
It generates all possible words that can be made from the letters of the master word.
Players submit guesses through the web interface.
The server checks the input and updates the game state dynamically.
