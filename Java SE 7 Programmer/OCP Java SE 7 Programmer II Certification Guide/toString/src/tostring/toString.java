package tostring;

class Book {
	String title;
	String isbn;
	String[] author;
	java.util.Date publishDate;
	double price;
	int version;
	String publisher;
	boolean eBookReady;
	
	@Override
	public String toString() {
		return title + ", ISBN:"+isbn + ", Lead Author:"+author[0];
	}
}

class toString {
	public static void main(String[] args) {
		Book b = new Book();
		b.title = "Java Smart Apps";
		b.author = new String[]{"Paul", "Larry"};
		b.isbn = "9810-9643-987";
		System.out.println(b);
	}
}
