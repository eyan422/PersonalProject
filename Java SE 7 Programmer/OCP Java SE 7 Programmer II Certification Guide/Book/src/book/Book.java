package book;

interface Printable {
	void print();
}

class ShoppingItem {
	public void description() {
		System.out.println("Shopping Item");
	}
}

public class Book extends ShoppingItem implements Printable {
	
	public void description() {
		System.out.println("Book");
	}
	
	public void print() {
		System.out.println("Printing book");
	}
	
	public static void main(String args[]) {
		Book book = new Book();
		
		Printable printable = book;
		printable.print();
		
		ShoppingItem shoppingItem = book;
		shoppingItem.description();
	}
}
