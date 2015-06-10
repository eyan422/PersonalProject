package interfaceandclass;

interface Employee {}

interface Printable extends Employee {
	String print();
}

class Programmer {
	public String print() { return("Programmer - Mala Gupta"); }
}

class Author extends Programmer implements Printable, Employee {
	public String print() { return("Author - Mala Gupta"); }
}

class InterfaceAndClass{
	public static void main()
	{
		Author a = new Author();
		a.print();
	}
}
