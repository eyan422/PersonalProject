package magazine;

class Book {
	private int pages = 100;
}
	
class Magazine extends Book {
	private int interviews = 2;
	public int pages = 100;
	
	private int totalPages()
	{
		/* INSERT CODE HERE */ 
		//System.out.println(this.pages+this.interviews);
		
		//System.out.println(pages+this.interviews);
		
		//System.out.println(super.pages+this.interviews);
		
		//return pages+this.interviews;
		return this.pages+this.interviews*5;
	}
	
	public static void main(String[] args) {
		System.out.println(new Magazine().totalPages());
	}
}
