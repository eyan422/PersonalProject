package testejava;

class Programmer 
{
	void print() 
	{
		System.out.println("Programmer - Mala Gupta");
	}
}

class Author extends Programmer 
{
	void print() 
	{
		System.out.println("Author - Mala Gupta");
	}
}

class TestEJava {
	public static void main(String args[]){
		Programmer a = new Programmer();
		Author b = new Author();
		//Programmer b = new Programmer();
		//Programmer b = new Author();
		//Author b = new Programmer();
		//Programmer b = ((Author)new Programmer());
		//Author b = ((Author)new Programmer());
		
		a.print();
		b.print();
	}
}

