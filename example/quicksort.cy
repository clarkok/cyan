function rand() : i64;
function print_int(value : i64);
function print_str(str : i8[]);

function swap(a : &i64, b : &i64) {
    let t = a;
    a = b;
    b = t;
}

function partition(a : i64[], lo : i64, hi : i64) : i64 {
    let pivot = a[hi];
    let i = lo;
    let j = lo;
    while (j < hi) {
        if (a[j] < pivot) {
            swap(a[i], a[j]);
            i = i + 1;
        }
        j = j + 1;
    }
    swap(a[i], a[j]);
    return i;
}

function quicksort(a : i64[], lo : i64, hi : i64) {
    if (lo < hi) {
        let p = partition(a, lo, hi);
        quicksort(a, lo, p - 1);
        quicksort(a, p + 1, hi);
    }
}

function main() {
    let n = 100;
    let a = new i64[n + 1];
    let i = 0;
    while (i < n) {
        a[i] = rand() % 1000;
        print_int(a[i]);
        print_str(" ");
        i = i + 1;
    }
    print_str("\n");

    quicksort(a, 0, n - 1);

    i = 0;
    while (i < n) {
        print_int(a[i]);
        print_str(" ");
        i = i + 1;
    }
    print_str("\n");
}
