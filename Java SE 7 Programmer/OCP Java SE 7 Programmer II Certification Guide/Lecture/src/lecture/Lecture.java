package lecture;

class Paper {}
class SmartPhone {}
class Laptop {}

class Lecture {
	void takeNotes(Paper paper) {}
	void takeNotes(SmartPhone phone) {}
	void takeNotes(Laptop laptop) {}
}

class Canvas {}

class FineArtLecture extends Lecture {
	void takeNotes(Canvas canvas) {}
}