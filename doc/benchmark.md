# Benchmarks

## vtebench

All benchmarks are done using [vtebench](https://github.com/alacritty/vtebench):

```sh
./target/release/vtebench -b ./benchmarks --dat /tmp/<terminal>
```

## 2021-06-25

### System

CPU: i9-9900

RAM: 64GB

Graphics: Radeon RX 5500XT


### Terminal configuration

Geometry: 2040x1884

Font: Fantasque Sans Mono 10.00pt/23px

Scrollback: 10000 lines


### Results

| Benchmark (times in ms)       | Foot (GCC+PGO) 1.9.2 | Foot 1.9.2 | Alacritty 0.9.0 | URxvt 9.26 | XTerm 369 |
|-------------------------------|---------------------:|-----------:|----------------:|-----------:|----------:|
| cursor motion                 |                13.69 |      15.63 |           29.16 |      23.69 |   1341.75 |
| dense cells                   |                40.77 |      50.76 |           92.39 |   13912.00 |   1959.00 |
| light cells                   |                 5.41 |       6.49 |           12.25 |      16.14 |     66.21 |
| scrollling                    |               125.43 |     133.00 |          110.90 |      98.29 |   4010.67 |
| scrolling bottom region       |               111.90 |     103.95 |          106.35 |     103.65 |   3787.00 |
| scrolling bottom small region |               120.93 |     112.48 |          129.61 |     137.21 |   3796.67 |
| scrolling fullscreen          |                 5.42 |       5.67 |           11.52 |      12.00 |    124.33 |
| scrolling top region          |               110.66 |     107.61 |          100.52 |     340.90 |   3835.33 |
| scrolling top small region    |               120.48 |     111.66 |          129.62 |     213.72 |   3805.33 |
| unicode                       |                10.19 |      11.27 |           14.72 |     787.77 |   4741.00 |


## 2022-05-12

### System

CPU: i5-8250U

RAM: 8GB

Graphics: Intel UHD Graphics 620


### Terminal configuration

Geometry: 945x1020

Font: Dina:pixelsize=12

Scrollback=10000 lines


### Results


| Benchmark (times in ms)       | Foot (GCC+PGO) 1.12.1 | Foot 1.12.1 | Alacritty 0.10.1 | URxvt 9.26 | XTerm 372 |
|-------------------------------|----------------------:|------------:|-----------------:|-----------:|----------:|
| cursor motion                 |                 15.03 |       16.74 |            23.22 |      24.14 |   1381.63 |
| dense cells                   |                 43.56 |       54.10 |            89.43 |    1807.17 |   1945.50 |
| light cells                   |                  7.96 |        9.66 |            20.19 |      21.31 |    122.44 |
| scrollling                    |                146.02 |      150.47 |           129.22 |     129.84 |  10140.00 |
| scrolling bottom region       |                138.36 |      137.42 |           117.06 |     141.87 |  10136.00 |
| scrolling bottom small region |                137.40 |      134.66 |           128.97 |     208.77 |   9930.00 |
| scrolling fullscreen          |                 11.66 |       12.02 |            19.69 |      21.96 |    315.80 |
| scrolling top region          |                143.81 |      133.47 |           132.51 |     475.81 |  10267.00 |
| scrolling top small region    |                133.72 |      135.32 |           145.10 |     314.13 |  10074.00 |
| unicode                       |                 20.89 |       21.78 |            26.11 |    5687.00 |  15740.00 |
