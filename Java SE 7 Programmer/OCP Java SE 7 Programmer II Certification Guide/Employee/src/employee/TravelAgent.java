package employee;

class Employee {}
class Engineer extends Employee {}
class CEO extends Employee {}

class Travel {
	static String bookTicket(Engineer val) {
		return "engineer class";
	}
	
	static String bookTicket(CEO val) {
		return "business class";
	}
	
	static String bookTicket(Employee val) {
		return "economy class";
	}
}

class TravelAgent {
	public static void main(String... args) {
		Employee emp = new CEO();
		System.out.println(Travel.bookTicket(emp));
		
		System.out.println(Travel.bookTicket(new CEO()));
	}
}
