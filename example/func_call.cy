function print_int(value : i64);
function print_str(str : i8[]);

function fact(n : i64) : i64 {
    if (n == 0) {
        return 0;
    }

    if (n == 1) {
        return 1;
    }

    return fact(n - 1) * n;
}

function main() {
    let i = 0;
    while (i < 10) {
        print_int(fact(i));
        print_str("\n");
        i = i + 1;
    }
}
