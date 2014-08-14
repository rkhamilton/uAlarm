This is the US cities15000 data from geonames.org. In my GPS location code I find the location closest to the clock's coordinates, and look up the timezone of that location, and whether it respects daylight savings time.

In order to fit the dataset into the memory of an Arduino Mega, I trimmed the dataset down by eliminating any locations which are far from the edges of a timezone border. When finding the closest city, it only matters that we find a location within the same timezone, so center city locations are redundant.

The code and locations used are in the uAlarm.ino file itself. I plan to refactor that functionality into a library later but for now it's just baked into the main project.