package testphones;

class Phone {
	static void call() {
		System.out.println("Call-Phone");
	}
}

class SmartPhone extends Phone{
	
	static void call() {
		System.out.println("Call-SmartPhone");
	}
}

class TestPhones {
	public static void main(String... args) {
		Phone phone = new Phone();
		Phone smartPhone = new SmartPhone();
		
		Phone.call();
		SmartPhone.call();
		
		phone.call();
		smartPhone.call();
		
	}
}
