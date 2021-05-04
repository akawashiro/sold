import myadd
import myobject

def test_myadd():
    assert myadd.myadd(1, 2) == 3

def test_myadd_keyword():
    assert myadd.myadd2(i=1, j=2) == 3

def test_myadd_default():
    assert myadd.myadd3() == 3

def test_myadd_const():
    assert myadd.the_answer == 42
    assert myadd.what == "World"

def test_myobject_pet():
    p = myobject.Pet('Molly')
    assert p.getName() == "Molly"
    p.setName('Charly')
    assert p.getName() == "Charly"

def test_myobject_dog():
    p = myobject.Dog('Molly')
    assert p.getName() == "Molly"
    assert p.bark() == "woof!"

def test_myobject_pet_store():
    p = myobject.pet_store()
    assert type(p).__name__ == "Pet"

def test_myobject_pet_store2():
    p = myobject.pet_store2()
    assert type(p).__name__ == "PolymorphicDog"
    assert p.bark() == "woof!"

def test_myobject_overload():
    p = myobject.Pet('Molly')
    p.set(10)
    p.set("hogehoge")
    assert p.getAge() == 10
    assert p.getName() == "hogehoge"

def test_myobject_enum():
    p = myobject.Pet('Lucy', myobject.Pet.Cat)
    assert p.type == 1
