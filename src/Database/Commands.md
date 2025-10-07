# run the docker container
docker run -d --name database-redis -p 6379:6379 database --appendonly yesA

# execute inside terminal to directly access redis (very fast short-term cache database)
docker exec -it database-redis redis-cli

# example commands inside redis CLI set and get
SET foo "bar"
GET foo


# #########################################

# execute inside terminal to directly access postgres (persistent database)
docker exec -it database-postgres psql -U atlas -d atlasmesh

# example commands inside postgres CLI to save data
CREATE TABLE test_table (
    id SERIAL PRIMARY KEY,
    name TEXT
);
INSERT INTO test_table (name) VALUES ('hello world');

# retrieve data in postgres
SELECT * FROM test_table;

# delete postgres volume
docker rm -f database-postgres
docker volume rm postgres_data
