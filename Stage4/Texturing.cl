

// apply computed checkerboard texture
Colour applyCheckerboard(const Intersection* intersect)
{
	Point p = (intersect->pos - intersect->material->offset) / intersect->material->size;

	int which = ((int)(floor(p.x)) + (int)(floor(p.y)) + (int)(floor(p.z))) & 1;

	return (which ? intersect->material->diffuse : intersect->material->diffuse2);
}


// apply computed circular texture
Colour applyCircles(const Intersection* intersect)
{
	Point p = (intersect->pos - intersect->material->offset) / intersect->material->size;

	int which = (int)(floor(sqrt(p.x * p.x + p.y * p.y + p.z * p.z))) & 1;

	return (which ? intersect->material->diffuse : intersect->material->diffuse2);
}


// apply computed wood grain texture
Colour applyWood(const Intersection* intersect)
{
	Point p = (intersect->pos - intersect->material->offset) / intersect->material->size;

	Point pTemp = p;
	// squiggle up where the point is
	p.x = (pTemp.x * cos(pTemp.y * 0.996f) * sin(pTemp.z * 1.023f));
	p.y = (cos(pTemp.x) * pTemp.y * sin(pTemp.z * 1.211f));
	p.z = (cos(pTemp.x * 1.473f)* cos(pTemp.y * 0.795f)* p.z );
	int which = (int)(floor(sqrt(p.x * p.x + p.y * p.y + p.z * p.z))) & 1;

	return (which ? intersect->material->diffuse : intersect->material->diffuse2);
}
