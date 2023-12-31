# Simple fulltext search index (in JS) + crawler (in C++)

Code is really messy and it will probably explode, so don't use it :)

## Building

```bash
git clone --recursive https://github.com/hanickadot/search-engine.git search-engine
cd search-engine
cmake -B build .
cmake --build build
./build/build-index https://urlA https://urlB
```

This will crawle all urls provided and links on same servers. And build index in `web/index` directory.

## Using index

Publish `web/` somewhere on web or locally (using [server.py](web/server.py)) and open browser and type what you search for.

### Searching phrases

Put what you search into double quotes: `"searching phrase"`

### Hyperlink targets

You can go directly to closest hyperlink target in documents by clicking on hashtags next to links in search results.

### Sharing search result with someone

Just copy address of the page including fragment and share it to someone :)
