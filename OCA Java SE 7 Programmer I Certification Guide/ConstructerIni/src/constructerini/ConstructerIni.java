package constructerini;

//public class ConstructerIni {
class Employee {
	String name;
	int age;
	
	Employee() 
	{
		//this ();
		this("Frank", 12);
	}
	
	private Employee (String newName, int newAge) 
	{
		name = newName;
		age = newAge;
	}
	
	{
		System.out.println("ini block");
	}
	
	void test(int i)
	{
		System.out.println("i");
	}
	
	void test(float l)
	{
		System.out.println("f");
	}
	
	void test(double l)
	{
		System.out.println("d");
	}
	
	void test(byte l)
	{
		System.out.println("b");
	}
	
	void test(long l)
	{
		System.out.println("l");
	}
	
	double calcAverage(double marks1, int marks2) {
		return (marks1 + marks2)/2.0;
	}
	
	double calcAverage(int marks1, double marks2) {
		return (marks1 + marks2)/2.0;
	}
	
	/*
	char[] test()
	{
		char x[] = null;
		return x;
	}
	*/
}

class ConstructerIni
{
	
	public static void main(String args[])
	{
		Employee e = new Employee();
		System.out.println(e.name);
		System.out.println(e.age);
		
		e.test(10);
		e.test(1.0f);
		
		//e.calcAverage(2, 3);
	}
}