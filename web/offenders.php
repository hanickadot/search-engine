<?php
	$dir = new DirectoryIterator($path = "index/leaves");
	$files = array();
	foreach ($dir as $fileinfo) {
		if (!$fileinfo->isDot()) {
			$name = $fileinfo->getFilename();
			$fs = filesize("$path/$name");
			$files[] = (object)array("name" => $name, "size" => $fs);
		}
	}
	
	uasort($files, function ($lhs, $rhs) {
		return $lhs->size - $rhs->size;
	});
	
	$count = count($files);
	//echo "count = $count\n";
	$percentile = $count / 100;
	
	$counter = 0;
	$n = 0;
	
	$histogram[0] = array();
	
	foreach ($files as $key => $value) {
		if (++$counter >= $percentile) {
			$histogram[++$n] = array();
			$counter = 0;
		}
		$histogram[$n][$value->name] = $value->size; 
	}
	
	for ($i = 0; $i != 95; ++$i) {
		unset($histogram[$i]);
	}
	
	$output = array();
	
	if (!function_exists('array_key_first')) {
		function array_key_first(array $arr) {
			foreach($arr as $key => $unused) return $key;
		}
	}
	
	if( !function_exists('array_key_last') ) {
		function array_key_last(array $array) {
			if( !empty($array) ) return key(array_slice($array, -1, 1, true));
		}
	}
	
	foreach ($histogram as $percent => $content) {
		$first = $content[array_key_first($content)];
		$last = $content[array_key_last($content)];
		$count = count($content);
		foreach ($content as $name => $size) {
			$output[str_replace(".json", "", $name)] = ($percent - 95);
		}
		
		//echo "$percent %: {$first}..{$last} count = {$count}\n";
	}
	ksort($output);
	
	$output = (object)$output;
	
	echo json_encode($output);
	
?>