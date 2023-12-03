set(SOURCES
    # DHT.hpp
    MatchesDHT.hpp
    MatchesDHT.cpp
    kvstore.cpp
)

add_grappa_application(kvstore ${SOURCES})