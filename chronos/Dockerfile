FROM alpine

RUN apk --no-cache add cmake build-base python3

WORKDIR /app

COPY *.cpp *.hpp CMakeLists.txt /app/

RUN cmake . && \
    make tests

CMD ./tests