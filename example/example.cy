concept Person {
    function getID() : i32;
    function getAge() : i32;
    function getName() : i8[];
}

struct Student {
    name: i8[],
    id: i32,
    age: i32
}

impl Student : Person {
    function getID() : i32 {
        return this.id;
    }

    function getAge() : i32 {
        return this.age;
    }

    function getName() : i8[] {
        return this.name;
    }
}

function newStudent(name : i8[], id : i32, age : i32) : Student {
    let ret = new Student;
    ret.name = name;
    ret.id = id;
    ret.age = age;

    return ret;
}

function deletePerson(target : Person) {
    delete target;
}

function main() {
    let name = new i8[10];
    let student = newStudent(name, 1, 2);
    student.Person.getName();
}
