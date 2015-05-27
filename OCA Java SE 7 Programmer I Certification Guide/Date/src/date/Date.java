package date;

class Phone{
	String model;
	String todaysDate() {
		return new java.util.Date().toString();
	}
}

class Date{
	public static void main(String... args)
	{
		Phone p = new Phone();
		System.out.println(p.todaysDate());
	}
}