## Ubuntu
Install the database
```
sudo apt install postgresql
```
Grant some permissions to the user
```
sudo -i -u postgres psql
```
Create a new user joe capable of creating databases
```
CREATE USER joe WITH LOGIN;
ALTER USER joe CREATEDB;
CREATE DATABASE joe OWNER joe;
```
Given new permissions, let's create the database
```
CREATE DATABASE road_n_roll;
```