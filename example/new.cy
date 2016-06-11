struct Student {
    id : u64
}

function main() : u64 {
    let student = new Student;
    student.id = 10;
    delete student;

    return 1;
}
