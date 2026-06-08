:i count 8
:b shell 46
echo 'seven' | ./build/epsl examples/dict.epsl
:i returncode 0
:b stdout 328
zero: 0
two: 2
seven: 7
three: 3
one: 1
nine: 9
six: 6
four: 4
eight: 8
five: 5
{"zero": 0, "two": 2, "seven": 7, "three": 3, "one": 1, "nine": 9, "six": 6, "four": 4, "eight": 8, "five": 5}
enter digit key: digit: 7
enter digit key: 
examples/dict.epsl:23:8: runtime error: no input
key := input("enter digit key: ");
       ^

:b stderr 0

:b shell 43
echo '10' | ./build/epsl examples/fact.epsl
:i returncode 0
:b stdout 22
enter x: x! = 3628800

:b stderr 0

:b shell 52
echo '10 + 5 * 3' | ./build/epsl examples/lexer.epsl
:i returncode 0
:b stdout 145
Enter expression: {.data: 10, .kind: .INT}
{.data: "+", .kind: .PLUS}
{.data: 5, .kind: .INT}
{.data: "*", .kind: .STAR}
{.data: 3, .kind: .INT}

:b stderr 0

:b shell 31
./build/epsl examples/json.epsl
:i returncode 0
:b stdout 3159
{
    "nested": {
        "level1": {
            "level2": {
                "level3": {
                    "array": [
                        1,
                        2,
                        3,
                        {
                            "inner": "object"
                        }
                    ],
                    "value": "deep"
                }
            }
        }
    },
    "logs": [
        {
            "timestamp": "2026-01-01T10:00:00Z",
            "level": "info",
            "message": "System started"
        },
        {
            "timestamp": "2026-01-01T10:05:00Z",
            "level": "warn",
            "message": "High memory usage"
        },
        {
            "timestamp": "2026-01-01T10:10:00Z",
            "level": "error",
            "message": "Crash detected"
        }
    ],
    "config": {
        "site_name": "Test Platform",
        "version": "1.0.0",
        "features": {
            "payments": false,
            "auth": true,
            "analytics": true
        }
    },
    "products": [
        {
            "tags": [
                "electronics",
                "computers"
            ],
            "name": "Laptop",
            "id": 101,
            "price": 999.990000
        },
        {
            "tags": [
                "electronics",
                "mobile"
            ],
            "name": "Phone",
            "id": 102,
            "price": 499.490000
        },
        {
            "tags": [
                "peripherals"
            ],
            "name": "Keyboard",
            "id": 103,
            "price": 79.950000
        }
    ],
    "users": [
        {
            "name": "Alice",
            "email": "alice@example.com",
            "roles": [
                "admin",
                "user"
            ],
            "id": 1,
            "active": true
        },
        {
            "name": "Bob",
            "email": "bob@example.com",
            "roles": [
                "user"
            ],
            "id": 2,
            "active": false
        },
        {
            "name": "Charlie",
            "email": "charlie@example.com",
            "roles": [
                "moderator",
                "user"
            ],
            "id": 3,
            "active": true
        }
    ],
    "orders": [
        {
            "id": 5001,
            "items": [
                {
                    "qty": 1,
                    "product_id": 101
                },
                {
                    "qty": 2,
                    "product_id": 103
                }
            ],
            "user_id": 1,
            "total": 1159.890000
        },
        {
            "id": 5002,
            "items": [
                {
                    "qty": 3,
                    "product_id": 102
                }
            ],
            "user_id": 2,
            "total": 1498.470000
        }
    ]
}

first log message: System started

last order: {
    "id": 5002,
    "items": [
        {
            "qty": 3,
            "product_id": 102
        }
    ],
    "user_id": 2,
    "total": 1498.470000
}

:b stderr 0

:b shell 31
./build/epsl examples/list.epsl
:i returncode 0
:b stdout 120
[1, 2, 3, 4, 5]
[1, 777, 3, 4, 5]
[1, 3, 4, 5]
[101, 1, 3, 4, 5]
{ 101 1 3 4 5 }
has 3: true
has 0: false
has 101: true

:b stderr 0

:b shell 37
./build/epsl examples/mandelbrot.epsl
:i returncode 0
:b stdout 2850
                                                                                              
                                                         ..                                   
                                                           ..                                 
                                                       . .,@,.                                
                                                       .@@@@@@i.                              
                                                       i@@@@@@,                               
                                            .,..  ox.;@lO@#@@ll@:..i      .                   
                                             ,@@@@@@@@@@@@@@@@@@@@@@@.,@l@.                   
                                          :...@@@@@@@@@@@@@@@@@@@@@@@@@@@.                    
                                         .,@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@:                    
                       .      ,         ,@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@,.                
                         .l;.,.@;.:.   .;@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@.                  
                        ..@@@@@@@@@@xl..@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@:.                 
                   .   .:@@@@@@@@@@@@@@,@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@;.                  
                  ..,@@:@@@@@@@@@@@@@@@l@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@;                    
        .       ...,@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@                     
                   .....@@@@@@@@@@@@@@@:@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@:                   
                       ,:,@@@@@@@@@@@i..@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@.                  
                         .;@,:@@@@@,. ..:@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@.                  
                        ...   .,        .i@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@.                 
                       .                 ..;@@@@@@@@@@@@@@@@@@@@@@@@@@@@@:.                   
                                         .;..@@@@@@@@@@@@@@@@@@@@@@@@@@@.                     
                                            .l@@;@@@@@@@@@@@@@@@@@@@@@O@@;                    
                                            :,o,o..@,@@@@@@@@@@@@@@,      .                   
                                              .        .@@@@@@.                               
                                                       .@@@@@@.                               
                                                      .,.,,@,o.,                              
                                                          .,.                                 
                                                          .   .                               
                                                         .                                    

:b stderr 0

:b shell 33
./build/epsl examples/object.epsl
:i returncode 0
:b stdout 156
{.name: "Michael", .address: {.city: "Moscow", .postcode: 832103}, .salary: 78561}

Name: Michael
Salary: 78561
Address: 
  City: Moscow
  Postcode: 832103

:b stderr 0

:b shell 32
./build/epsl examples/runes.epsl
:i returncode 0
:b stdout 140
Original string: Привет, Мир!
Length: 12
After changing: Привет, Мир?!

Printing runes: Прив
'0' + 3 = '3'

1055 1055

:b stderr 0

