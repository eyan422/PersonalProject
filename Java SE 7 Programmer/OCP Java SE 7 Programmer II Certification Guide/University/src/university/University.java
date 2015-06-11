package university;

class Course {
	String title;
	Course(String title) {
		this.title = title;
	}
	public boolean equals(Object o) {
		return (title.equals(o));
	}
}

class University {
	public static void main(String[] args) {
		Course c1 = new Course("level1");
		String s1 = "level1";
		System.out.println(c1.equals(s1));
		System.out.println(s1.equals(c1));
		System.out.println(c1.equals(c1));
	}
}
