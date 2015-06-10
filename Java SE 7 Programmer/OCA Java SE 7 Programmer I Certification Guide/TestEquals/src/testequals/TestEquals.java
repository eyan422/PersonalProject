package testequals;

class Book {
	String ISBN;
	
	Book(String val) {
		ISBN = val;
	}
	
	public boolean equals(Object b) {
		if (b instanceof Book) {
		return ((Book)b).ISBN.equals(ISBN);
		}
		else
		return false;
	}
}

class TestEquals {
	public static void main(String args[]) {
		Book b1 = new Book("1234-4657");
		Book b2 = new Book("1234-4657");
		System.out.print(b1.equals(b2) +":");
		System.out.print(b1 == b2);
	}
}
