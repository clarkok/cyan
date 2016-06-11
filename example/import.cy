function print_str(str : i8[]);
function print_int(value : i64);
function rand() : i32;

function main() {
    print_str("The random number is: ");
    print_int(rand());
    print_str("\n");
}
