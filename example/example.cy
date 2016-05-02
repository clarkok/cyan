
conecpt Person
{
    fn const getName() : string;
    fn const getAge() : i32;
};

struct Student
{
    string name;
    i32 age;
};

impl Persion for Student
{
    fn const
    getName() : string
    { return name; }

    fn const
    getAge() : i32
    { return age; }
};

fn
printPersionInfo(Person p) : void
{
    print(p.getName());
    println(p.getAge());
}

fn
main()
{
    let person = new Student { name: "Clarkok", age: 21 };
    printPersonInfo(person);
}

