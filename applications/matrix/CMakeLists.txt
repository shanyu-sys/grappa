set(SOURCES
    grappa_matmul.cpp
    Matrix.cpp
    Matrix.hpp
)

add_grappa_application(matmul.exe ${SOURCES})