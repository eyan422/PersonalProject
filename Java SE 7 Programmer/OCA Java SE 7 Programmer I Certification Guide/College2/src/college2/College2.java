package college2;

class Phone {
String keyboard = "in-built";
}

class Tablet extends Phone {
boolean playMovie = false;
}

class College2 {
		public static void main(String args[]) {
		Phone phone = new Tablet();
		System.out.println(phone.keyboard);
		
		System.out.println(((Tablet)phone).playMovie);
	}
}
