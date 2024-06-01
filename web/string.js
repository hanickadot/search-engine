// add ngrams() to String type
Array.prototype.slice_length = function(index, length) {
	return this.slice(index, index + length);
}

Array.prototype.slice_off_last = function(length) {
	return this.slice(0, -length + 1);
}

Array.prototype.adjacent_transform = function(length, cb) {
	return this.map((_, i) => cb(this.slice_length(i, length))).slice_off_last(length);
}

Array.prototype.to_hexdec = function() {
  return this.reduce((acc, current) => acc.concat(current.toString(16).padStart(2, '0')), "");
}

String.prototype.ngrams = function(length = 3) {
	const bytes = (new TextEncoder()).encode(this);
	return [...bytes].adjacent_transform(length, arr => arr.to_hexdec());
}

String.prototype.copy = function() {
	return JSON.parse(JSON.stringify(this));
}

String.prototype.split_to_words = function() {
	let i = 0;
	let word = "";
	let output = [];
	
	let in_quotes = false;
	
	const state_type = {
		text: 0,
		quotes: 1,
		double_quotes: 2
	};
	
	let state = state_type.text;
	
	for (let i = 0; i != this.length; ++i) {
		const c = this[i];
		
		if (state == state_type.text) {
			if (c == ' ' && word.length == 0) {
				continue;
			} else if (c == ' ' && word.length > 0) {
				output.push(word);
				word = "";
				continue;
			} else if (c == '"' && (word.length == 0 || word == "-")) {
				state = state_type.double_quotes
				continue;
			} else if (c == "'" && (word.length == 0 || word == "-")) {
				state = state_type.quotes
				continue;
			} else {
				word += c;
			}
		} else if (state == state_type.quotes) {
			if (c == "'") {
				state = state_type.text;
				continue;
			} else {
				word += c;
			}
			
		} else if (state == state_type.double_quotes) {
			if (c == '"') {
				state = state_type.text;
				continue;
			} else {
				word += c;
			}
		}
	}
	if (word.length > 0) {
		output.push(word);
	}
	return output;
}
