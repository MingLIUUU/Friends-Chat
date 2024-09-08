# Friends-Chat

A simple yet powerful chat software that makes communication between friends more convenient.

## Project Overview

Friends-Chat is a chat software written in C, designed to provide a lightweight and easy-to-use platform for real-time communication between friends.

## Key Features

- Real-time message delivery
- User-friendly simple interface
- Multi-user support
- Secure data transmission

## Installation Guide

1. Clone the repository:
   ```
   git clone https://github.com/your-username/Friends-Chat.git
   ```
2. Navigate to the project directory:
   ```
   cd Friends-Chat
   ```
3. Compile the project:
   ```
   make
   ```
4. Run the server, with a port number of your choice, we use 56403 for example:
   ```
   ./friend_server 56403
   ```
5. Run the client in another terminal:
   ```
   nc localhost 56403
   ```

## Tech Stack

- C programming language
- Socket programming

## Commands

1. Enter the username to login, enter "quit" to exit:
   ```
   quit
   ```
2. Enter "list_users" to list all users:
   ```
   list_users
   ```
3. Enter "make_friends [target]" to make friends with another user:
   ```
   make_friends Bob
   ```
4. Enter "post [target] [msgpiece ...]" to post a message to a friend:
   ```
   post Bob Hello, how are you today?
   ```

## Acknowledgements

This project was developed as part of an assessment for the 2023 CSC209: Software Tools and Systems Programming course at the University of Toronto, Canada. We thank everyone who has contributed to this project.

## License

This project is licensed under the [MIT License](LICENSE).

## Contact

If you have any questions or suggestions, please contact us through GitHub issues.

