package testphones;

class Phone {
	static void call() {
		System.out.println("Call-Phone");
	}
	
	void show() {
		System.out.println("Phone");
	}
}

class SmartPhone extends Phone{
	
	static void call() {
		System.out.println("Call-SmartPhone");
	}
	
	void show() {
		System.out.println("SmartPhone");
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
		
		phone.show();
		smartPhone.show();
	}
}
