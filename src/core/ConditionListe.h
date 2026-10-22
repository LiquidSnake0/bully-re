// Une liste de conditions, comme FUN_005fb9b0 la juge (celle d'un nœud d'action, d'une mission, ou d'un groupe :
// ConditionGroup). Chaque élément est une condition (genre 0), un NOT (genre 1) ou un OR (genre 2), dans l'ordre :
//   - une condition après une condition (ou en tête) donne le résultat ; après un NOT, son contraire ;
//   - un OR après une condition fausse est sauté : la condition suivante décide ; après une vraie, la condition
//     qui suit l'OR est encore jugée, mais sa valeur ne compte pas (seule une condition inconnue y change tout) ;
//   - le premier résultat faux qui n'est pas suivi d'un OR rend la liste fausse.
// juge(i) : 1 vrai, 0 faux, -1 inconnu (la liste devient inconnue) ; genre(i) : 0, 1 ou 2.
#pragma once

template<class Juge, class Genre>
int
EvaluerListe(int n, Juge juge, Genre genre)
{
	bool resultat = true, saute = false;
	int precedent = 0;
	for(int i = 0; i < n; i++){
		if(!saute){
			int v = juge(i);
			if(v < 0) return -1;
			if(precedent == 0) resultat = v != 0;
			else if(precedent == 1) resultat = v == 0;
			else saute = true;
		}
		int g = genre(i);
		precedent = g;
		if(saute && g != 0){ saute = false; continue; }
		if(resultat) continue;
		if(i + 1 >= n) break;
		if(genre(i + 1) != 2) break;
		i++;                                         // le OR qui suit un faux est sauté
	}
	return resultat;
}
