package result;

class Student
{
	int marks = 10;
}

class Result {
	public static void main(String... args) {
		Student s = new Student();
		
		switch (s.marks) {
			default: System.out.println("100");
			case 10: System.out.println("10");
			case 98: System.out.println("98");
		}
	}
}
